/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/dpms.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/registry.h"
#include "../../src/server/display.h"
#include "../../src/server/dpms_interface.h"
#include "../../src/server/output_interface.h"
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
    void testModeChanges();
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
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_serverOutput = new OutputInterface(m_display, this);
    QCOMPARE(m_serverOutput->pixelSize(), QSize());
    QCOMPARE(m_serverOutput->refreshRate(), 60000);
    m_serverOutput->addMode(QSize(800, 600), OutputInterface::ModeFlags(OutputInterface::ModeFlag::Preferred));
    QCOMPARE(m_serverOutput->pixelSize(), QSize(800, 600));
    m_serverOutput->addMode(QSize(1024, 768));
    m_serverOutput->addMode(QSize(1280, 1024), OutputInterface::ModeFlags(), 90000);
    QCOMPARE(m_serverOutput->pixelSize(), QSize(800, 600));
    m_serverOutput->setCurrentMode(QSize(1024, 768));
    QCOMPARE(m_serverOutput->pixelSize(), QSize(1024, 768));
    QCOMPARE(m_serverOutput->refreshRate(), 60000);
    m_serverOutput->create();
    QCOMPARE(m_serverOutput->isDpmsSupported(), false);
    QCOMPARE(m_serverOutput->dpmsMode(), OutputInterface::DpmsMode::Off);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, SIGNAL(connected()));
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

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, SIGNAL(outputAnnounced(quint32,quint32)));
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

    QSignalSpy outputChanged(&output, SIGNAL(changed()));
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

void TestWaylandOutput::testModeChanges()
{
    // verify the server modes
    using namespace KWaylandServer;
    const auto serverModes = m_serverOutput->modes();
    QCOMPARE(serverModes.count(), 3);
    QCOMPARE(serverModes.at(0).size, QSize(800, 600));
    QCOMPARE(serverModes.at(1).size, QSize(1024, 768));
    QCOMPARE(serverModes.at(2).size, QSize(1280, 1024));
    QCOMPARE(serverModes.at(0).refreshRate, 60000);
    QCOMPARE(serverModes.at(1).refreshRate, 60000);
    QCOMPARE(serverModes.at(2).refreshRate, 90000);
    QCOMPARE(serverModes.at(0).flags, OutputInterface::ModeFlags(OutputInterface::ModeFlag::Preferred));
    QCOMPARE(serverModes.at(1).flags, OutputInterface::ModeFlags(OutputInterface::ModeFlag::Current));
    QCOMPARE(serverModes.at(2).flags, OutputInterface::ModeFlags());

    using namespace KWayland::Client;
    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, SIGNAL(outputAnnounced(quint32,quint32)));
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, SIGNAL(changed()));
    QVERIFY(outputChanged.isValid());
    QSignalSpy modeAddedSpy(&output, SIGNAL(modeAdded(KWayland::Client::Output::Mode)));
    QVERIFY(modeAddedSpy.isValid());
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(modeAddedSpy.count(), 3);
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().size, QSize(800, 600));
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Preferred));
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().output, QPointer<Output>(&output));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().refreshRate, 90000);
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::None));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().output, QPointer<Output>(&output));
    QCOMPARE(modeAddedSpy.at(2).first().value<Output::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeAddedSpy.at(2).first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(2).first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(2).first().value<Output::Mode>().output, QPointer<Output>(&output));
    const QList<Output::Mode> &modes = output.modes();
    QCOMPARE(modes.size(), 3);
    QCOMPARE(modes.at(0), modeAddedSpy.at(0).first().value<Output::Mode>());
    QCOMPARE(modes.at(1), modeAddedSpy.at(1).first().value<Output::Mode>());
    QCOMPARE(modes.at(2), modeAddedSpy.at(2).first().value<Output::Mode>());

    QCOMPARE(output.pixelSize(), QSize(1024, 768));

    // change the current mode
    outputChanged.clear();
    QSignalSpy modeChangedSpy(&output, &KWayland::Client::Output::modeChanged);
    QVERIFY(modeChangedSpy.isValid());
    m_serverOutput->setCurrentMode(QSize(800, 600));
    QVERIFY(modeChangedSpy.wait());
    if (modeChangedSpy.size() == 1) {
        QVERIFY(modeChangedSpy.wait());
    }
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().flags, Output::Mode::Flags());
    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Current | Output::Mode::Flag::Preferred));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(800, 600));
    const QList<Output::Mode> &modes2 = output.modes();
    QCOMPARE(modes2.at(0).size, QSize(1280, 1024));
    QCOMPARE(modes2.at(0).refreshRate, 90000);
    QCOMPARE(modes2.at(0).flags, Output::Mode::Flag::None);
    QCOMPARE(modes2.at(1).size, QSize(1024, 768));
    QCOMPARE(modes2.at(1).refreshRate, 60000);
    QCOMPARE(modes2.at(1).flags, Output::Mode::Flag::None);
    QCOMPARE(modes2.at(2).size, QSize(800, 600));
    QCOMPARE(modes2.at(2).refreshRate, 60000);
    QCOMPARE(modes2.at(2).flags, Output::Mode::Flag::Current | Output::Mode::Flag::Preferred);

    // change once more
    outputChanged.clear();
    modeChangedSpy.clear();
    m_serverOutput->setCurrentMode(QSize(1280, 1024), 90000);
    QCOMPARE(m_serverOutput->refreshRate(), 90000);
    QVERIFY(modeChangedSpy.wait());
    if (modeChangedSpy.size() == 1) {
        QVERIFY(modeChangedSpy.wait());
    }
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Preferred));
    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().refreshRate, 90000);
    QCOMPARE(modeChangedSpy.last().first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Current));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(1280, 1024));
}

void TestWaylandOutput::testScaleChange()
{
    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, SIGNAL(outputAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, SIGNAL(changed()));
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
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 4);
}

void TestWaylandOutput::testSubPixel_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QTest::addColumn<KWayland::Client::Output::SubPixel>("expected");
    QTest::addColumn<KWaylandServer::OutputInterface::SubPixel>("actual");

    QTest::newRow("none") << Output::SubPixel::None << OutputInterface::SubPixel::None;
    QTest::newRow("horizontal/rgb") << Output::SubPixel::HorizontalRGB << OutputInterface::SubPixel::HorizontalRGB;
    QTest::newRow("horizontal/bgr") << Output::SubPixel::HorizontalBGR << OutputInterface::SubPixel::HorizontalBGR;
    QTest::newRow("vertical/rgb") << Output::SubPixel::VerticalRGB << OutputInterface::SubPixel::VerticalRGB;
    QTest::newRow("vertical/bgr") << Output::SubPixel::VerticalBGR << OutputInterface::SubPixel::VerticalBGR;
}

void TestWaylandOutput::testSubPixel()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QFETCH(OutputInterface::SubPixel, actual);
    QCOMPARE(m_serverOutput->subPixel(), OutputInterface::SubPixel::Unknown);
    QSignalSpy serverSubPixelChangedSpy(m_serverOutput, &OutputInterface::subPixelChanged);
    QVERIFY(serverSubPixelChangedSpy.isValid());
    m_serverOutput->setSubPixel(actual);
    QCOMPARE(m_serverOutput->subPixel(), actual);
    QCOMPARE(serverSubPixelChangedSpy.count(), 1);
    // changing to same value should not trigger the signal
    m_serverOutput->setSubPixel(actual);
    QCOMPARE(serverSubPixelChangedSpy.count(), 1);

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, SIGNAL(outputAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, SIGNAL(changed()));
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }

    QTEST(output.subPixel(), "expected");

    // change back to unknown
    outputChanged.clear();
    m_serverOutput->setSubPixel(OutputInterface::SubPixel::Unknown);
    QCOMPARE(m_serverOutput->subPixel(), OutputInterface::SubPixel::Unknown);
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
    QTest::addColumn<KWaylandServer::OutputInterface::Transform>("actual");

    QTest::newRow("90")          << Output::Transform::Rotated90  << OutputInterface::Transform::Rotated90;
    QTest::newRow("180")         << Output::Transform::Rotated180 << OutputInterface::Transform::Rotated180;
    QTest::newRow("270")         << Output::Transform::Rotated270 << OutputInterface::Transform::Rotated270;
    QTest::newRow("Flipped")     << Output::Transform::Flipped    << OutputInterface::Transform::Flipped;
    QTest::newRow("Flipped 90")  << Output::Transform::Flipped90  << OutputInterface::Transform::Flipped90;
    QTest::newRow("Flipped 180") << Output::Transform::Flipped180 << OutputInterface::Transform::Flipped180;
    QTest::newRow("Flipped 280") << Output::Transform::Flipped270 << OutputInterface::Transform::Flipped270;
}

void TestWaylandOutput::testTransform()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QFETCH(OutputInterface::Transform, actual);
    QCOMPARE(m_serverOutput->transform(), OutputInterface::Transform::Normal);
    QSignalSpy serverTransformChangedSpy(m_serverOutput, &OutputInterface::transformChanged);
    QVERIFY(serverTransformChangedSpy.isValid());
    m_serverOutput->setTransform(actual);
    QCOMPARE(m_serverOutput->transform(), actual);
    QCOMPARE(serverTransformChangedSpy.count(), 1);
    // changing to same should not trigger signal
    m_serverOutput->setTransform(actual);
    QCOMPARE(serverTransformChangedSpy.count(), 1);

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, SIGNAL(outputAnnounced(quint32,quint32)));
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output *output = registry.createOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>(), &registry);
    QSignalSpy outputChanged(output, SIGNAL(changed()));
    QVERIFY(outputChanged.isValid());
    wl_display_flush(m_connection->display());
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }

    QTEST(output->transform(), "expected");

    // change back to normal
    outputChanged.clear();
    m_serverOutput->setTransform(OutputInterface::Transform::Normal);
    QCOMPARE(m_serverOutput->transform(), OutputInterface::Transform::Normal);
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
    QTest::addColumn<KWaylandServer::OutputInterface::DpmsMode>("server");

    QTest::newRow("Standby") << Dpms::Mode::Standby << OutputInterface::DpmsMode::Standby;
    QTest::newRow("Suspend") << Dpms::Mode::Suspend << OutputInterface::DpmsMode::Suspend;
    QTest::newRow("On") << Dpms::Mode::On << OutputInterface::DpmsMode::On;
}

void TestWaylandOutput::testDpms()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    new DpmsManagerInterface(m_display);

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

    Output *output = registry.createOutput(registry.interface(Registry::Interface::Output).name, registry.interface(Registry::Interface::Output).version, &registry);

    DpmsManager *dpmsManager = registry.createDpmsManager(dpmsAnnouncedSpy.first().first().value<quint32>(), dpmsAnnouncedSpy.first().last().value<quint32>(), &registry);
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
    QSignalSpy serverDpmsModeChangedSpy(m_serverOutput, &OutputInterface::dpmsModeChanged);
    QVERIFY(serverDpmsModeChangedSpy.isValid());
    QSignalSpy clientDpmsModeChangedSpy(dpms, &Dpms::modeChanged);
    QVERIFY(clientDpmsModeChangedSpy.isValid());

    QCOMPARE(m_serverOutput->dpmsMode(), OutputInterface::DpmsMode::Off);
    QFETCH(OutputInterface::DpmsMode, server);
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
    m_serverOutput->setDpmsMode(OutputInterface::DpmsMode::Off);
    QVERIFY(clientDpmsModeChangedSpy.wait());
    QCOMPARE(clientDpmsModeChangedSpy.count(), 2);
    QCOMPARE(dpms->mode(), Dpms::Mode::Off);
}

void TestWaylandOutput::testDpmsRequestMode_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    QTest::addColumn<KWayland::Client::Dpms::Mode>("client");
    QTest::addColumn<KWaylandServer::OutputInterface::DpmsMode>("server");

    QTest::newRow("Standby") << Dpms::Mode::Standby << OutputInterface::DpmsMode::Standby;
    QTest::newRow("Suspend") << Dpms::Mode::Suspend << OutputInterface::DpmsMode::Suspend;
    QTest::newRow("Off") << Dpms::Mode::Off << OutputInterface::DpmsMode::Off;
    QTest::newRow("On") << Dpms::Mode::On << OutputInterface::DpmsMode::On;
}

void TestWaylandOutput::testDpmsRequestMode()
{
    // this test verifies that requesting a dpms change from client side emits the signal on server side
    using namespace KWayland::Client;
    using namespace KWaylandServer;

    // setup code
    new DpmsManagerInterface(m_display);

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

    Output *output = registry.createOutput(registry.interface(Registry::Interface::Output).name, registry.interface(Registry::Interface::Output).version, &registry);

    DpmsManager *dpmsManager = registry.createDpmsManager(dpmsAnnouncedSpy.first().first().value<quint32>(), dpmsAnnouncedSpy.first().last().value<quint32>(), &registry);
    QVERIFY(dpmsManager->isValid());

    Dpms *dpms = dpmsManager->getDpms(output, &registry);
    // and test request mode
    QSignalSpy modeRequestedSpy(m_serverOutput, &OutputInterface::dpmsModeRequested);
    QVERIFY(modeRequestedSpy.isValid());

    QFETCH(Dpms::Mode, client);
    dpms->requestMode(client);
    QVERIFY(modeRequestedSpy.wait());
    QTEST(modeRequestedSpy.last().first().value<OutputInterface::DpmsMode>(), "server");
}

QTEST_GUILESS_MAIN(TestWaylandOutput)
#include "test_wayland_output.moc"
